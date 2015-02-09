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

#include "gcs_corosync_communication_interface.h"
#include "gcs_corosync_utils.h"

cs_error_t
Gcs_corosync_communication_proxy_impl::cpg_mcast_joined
                                (cpg_handle_t handle,
                                 cpg_guarantee_t guarantee,
                                 const struct iovec* iovec,
                                 unsigned int iov_len)
{
  return ::cpg_mcast_joined(handle, guarantee, iovec, iov_len);
}

Gcs_corosync_communication::Gcs_corosync_communication
                              (cpg_handle_t handle,
                               Gcs_corosync_statistics_updater *stats,
                               Gcs_corosync_communication_proxy *proxy,
                               Gcs_corosync_view_change_control_interface *vce)
      : corosync_handle(handle), stats(stats), proxy(proxy), view_notif(vce)
{
}

Gcs_corosync_communication::~Gcs_corosync_communication()
{
}

map<int, Gcs_communication_event_listener*>*
Gcs_corosync_communication::get_event_listeners()
{
  return &event_listeners;
}

bool
Gcs_corosync_communication::send_message(Gcs_message *message_to_send)
{
  view_notif->wait_for_view_change_end();

  long message_length= this->send_binding_message(message_to_send);

  if(message_length != -1)
  {
    this->stats->update_message_sent(message_length);
  }

  return (message_length == -1);
}

long
Gcs_corosync_communication::send_binding_message(Gcs_message* message_to_send)
{
  vector<uchar>* encoded_message= message_to_send->encode();

  struct iovec iov;

  iov.iov_base= (void*) &encoded_message->front();
  iov.iov_len= encoded_message->size();

  int res= 0;
  GCS_COROSYNC_RETRIES(proxy->cpg_mcast_joined
                             (corosync_handle,
                              to_corosync_guarantee
                                      (message_to_send->
                                              get_delivery_guarantee()),
                              &iov, 1), res);
  delete encoded_message;

  long ret_value= (res != CS_OK) ? -1: (long) iov.iov_len;

  return ret_value;
}

int
Gcs_corosync_communication::add_event_listener
                            (Gcs_communication_event_listener *event_listener)
{
  //This construct avoid the clash of keys in the map
  int handler_key= 0;
  do {
    handler_key= rand();
  } while(event_listeners.count(handler_key) != 0);

  event_listeners[handler_key]= event_listener;

  return handler_key;
}

void
Gcs_corosync_communication::remove_event_listener(int event_listener_handle)
{
  event_listeners.erase(event_listener_handle);
}

void
Gcs_corosync_communication::deliver_message
                    (const struct cpg_name *name, uint32_t nodeid,
                     uint32_t pid, void *data, size_t len)
{
  Gcs_member_identifier* origin=
                     Gcs_corosync_utils::build_corosync_member_id(nodeid, pid);
  Gcs_group_identifier destination(string(name->value));

  Gcs_message message(*origin,
                      destination,
                      (gcs_message_delivery_guarantee)0);

  message.decode((uchar*)data, len);

  map<int, Gcs_communication_event_listener*>::iterator callback_it=
                                                      event_listeners.begin();
  while(callback_it != event_listeners.end())
  {
    callback_it->second->on_message_received(message);
    ++callback_it;
  }

  this->stats->update_message_received((long)len);

  delete origin;
}

cpg_guarantee_t
Gcs_corosync_communication::to_corosync_guarantee
                                    (gcs_message_delivery_guarantee param)
{
  cpg_guarantee_t mapped_value= CPG_TYPE_UNORDERED;

  switch(param)
  {

  case NO_ORDER:
    mapped_value= CPG_TYPE_UNORDERED;
    break;

  case TOTAL_ORDER:
    mapped_value= CPG_TYPE_AGREED;
    break;

  case UNIFORM:
    mapped_value= CPG_TYPE_SAFE;
    break;

  default:
    break;
  }

  return mapped_value;
}

