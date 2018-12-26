/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_xcom_communication_interface.h"

#include "gcs_message_stages.h"
#include "gcs_logging.h"

#include "simset.h"
#include "task.h"
#include "xcom_vp.h"
#include "server_struct.h"
#include "xcom_detector.h"
#include "site_struct.h"
#include "xcom_common.h"
#include "node_list.h"
#include "node_set.h"
#include "xcom_base.h"
#include "app_data.h"
#include "synode_no.h"
#include "node_no.h"
#include "pax_msg.h"
#include "xcom_transport.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>

#define NUMBER_OF_XCOM_SOCKET_RETRIES 1000

using std::map;


Gcs_xcom_communication::
Gcs_xcom_communication(Gcs_xcom_statistics_updater *stats,
                       Gcs_xcom_proxy *proxy,
                       Gcs_xcom_view_change_control_interface *view_control)
  : event_listeners(), stats(stats), m_xcom_proxy(proxy),
    m_view_control(view_control), m_msg_pipeline(), m_buffered_messages()
/* purecov: begin deadcode */
{ }
/* purecov: end */

Gcs_xcom_communication::~Gcs_xcom_communication()
{ }


std::map<int, const Gcs_communication_event_listener &> *
Gcs_xcom_communication::get_event_listeners()
{
  return &event_listeners;
}


enum_gcs_error
Gcs_xcom_communication::send_message(const Gcs_message &message_to_send)
{
  MYSQL_GCS_LOG_TRACE("Sending message.")

  unsigned long long message_length= 0;
  enum_gcs_error message_result= GCS_NOK;

  /*
    This is an optimistic attempt to avoid sending a message to a
    group when the node doesn't belong to it. If it is kicked out
    of the group while trying to send a message, this function
    should eventually return an error.
  */
  if (!m_view_control->belongs_to_group())
  {
    MYSQL_GCS_LOG_ERROR(
      "Message cannot be sent because the member does not belong to "
      "a group."
    )
    return GCS_NOK;
  }

  message_result= this->send_binding_message(message_to_send,
                                             &message_length,
                                             Gcs_internal_message_header::CT_USER_DATA);

  if (message_result == GCS_OK)
  {
    this->stats->update_message_sent(message_length);
  }

  return message_result;
}


enum_gcs_error
Gcs_xcom_communication::
send_binding_message(const Gcs_message &msg,
                     unsigned long long *msg_len,
                     Gcs_internal_message_header::enum_cargo_type cargo)
{
  unsigned long long msg_length= 0;
  enum_gcs_error ret= GCS_NOK;
  Gcs_message_data &msg_data= msg.get_message_data();
  unsigned long long len= msg_data.get_header_length() +
                          msg_data.get_payload_length();
  Gcs_packet packet(len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  uint64_t buffer_size= packet.get_capacity();
  unsigned long long payload_len;
  Gcs_internal_message_header gcs_header;
  if (packet.get_buffer() == NULL)
  {
    MYSQL_GCS_LOG_ERROR("Error generating the binding message.")
    goto end;
  }

  // insert the payload
  if (msg_data.encode(packet.get_buffer() + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE,
                      &buffer_size))
  {
    MYSQL_GCS_LOG_ERROR("Error inserting the payload in the binding message.")
    goto end;
  }

  payload_len= buffer_size;

  // Insert the header in the buffer
  gcs_header.set_msg_length(payload_len + Gcs_internal_message_header::WIRE_FIXED_HEADER_SIZE);
  gcs_header.set_dynamic_headers_length(0);
  gcs_header.set_cargo_type(cargo);
  gcs_header.encode(packet.get_buffer());

  // reload the header information into the packet
  packet.reload_header(gcs_header);

  MYSQL_GCS_LOG_TRACE("Pipelining message with payload length "
                      << packet.get_payload_length())

  // apply transformations
  if (m_msg_pipeline.outgoing(packet))
  {
    MYSQL_GCS_LOG_ERROR("Error preparing the message for sending.")
    goto end;
  }

  /*
    Note that XCom will own the packet buffer now, so we don't need to
    free it before exiting.
  */
  msg_length= packet.get_length();
  MYSQL_GCS_LOG_TRACE("Sending message with payload length " << msg_length)
  if (m_xcom_proxy->xcom_client_send_data(msg_length, reinterpret_cast<char *>(packet.get_buffer())))
  {
    MYSQL_GCS_LOG_ERROR(
      "Error pushing message into group communication engine."
    )
    goto end;
  }

  *msg_len= len;
  ret= GCS_OK;

end:
  if (ret == GCS_NOK)
    free(packet.get_buffer());

  MYSQL_GCS_LOG_TRACE(
    "send_binding_message enum_gcs_error result= " <<
    static_cast<unsigned int>(ret) <<
    ". Bytes sent:" << msg_length
  )

  return ret;
}


int
Gcs_xcom_communication::
add_event_listener(const Gcs_communication_event_listener &event_listener)
{
  // This construct avoid the clash of keys in the map
  int handler_key= 0;
  do
  {
    handler_key= rand();
  }
  while (event_listeners.count(handler_key) != 0);

  std::pair<int, const Gcs_communication_event_listener &>
    to_insert(handler_key,event_listener);

  event_listeners.insert(to_insert);

  return handler_key;
}


void Gcs_xcom_communication::remove_event_listener(int event_listener_handle)
{
  event_listeners.erase(event_listener_handle);
}


bool Gcs_xcom_communication::xcom_receive_data(Gcs_message *message)
{
  /*
    If a view exchange phase is being executed, messages are buffered
    and then delivered to the application after the view has been
    installed. This is done to avoid delivering messages to the
    application in nodes that are joining because it would be strange
    to receive messages before any view.

    We could have relaxed this a little bit and could have let nodes
    from an old view to immediately deliver messages. However, we
    don't do this because we want to provide virtual synchrony. Note
    that we don't guarantee that a message sent in a view will be
    delivered in the same view.

    It is also important to note that this method must be executed by
    the same thread that processes global view messages and data
    message in order to avoid any concurrency issue.
  */
  if (m_view_control->is_view_changing())
  {
     buffer_message(message);
     return false;
  }

  /*
    The node belongs to a group and is not executing the state
    exchange phase.
  */
  notify_received_message(message);

  return true;
}


void Gcs_xcom_communication::notify_received_message(Gcs_message *message)
{
  map<int, const Gcs_communication_event_listener &>::iterator callback_it=
    event_listeners.begin();

  while (callback_it != event_listeners.end())
  {
    callback_it->second.on_message_received(*message);

    MYSQL_GCS_LOG_TRACE(
      "Delivered message to client handler= "<< (*callback_it).first
    )

    ++callback_it;
  }

  stats->update_message_received((long)(message->get_message_data()
                                                   .get_header_length() +
                                        message->get_message_data()
                                                   .get_payload_length()));

  MYSQL_GCS_LOG_TRACE("Delivered message from origin= " <<
                      message->get_origin().get_member_id().c_str())

  delete message;
}


void Gcs_xcom_communication::buffer_message(Gcs_message *message)
{
  assert(m_view_control->is_view_changing());
  MYSQL_GCS_LOG_TRACE("Buffering message: " << message)
  m_buffered_messages.push_back(message);
}


void Gcs_xcom_communication::deliver_buffered_messages()
{
  std::vector<Gcs_message *>::iterator buffer_msg_it;

  for (buffer_msg_it= m_buffered_messages.begin();
       buffer_msg_it != m_buffered_messages.end();
       buffer_msg_it++)
  {
    MYSQL_GCS_LOG_TRACE("Delivering buffered message: " << *buffer_msg_it)
    notify_received_message(*buffer_msg_it);
  }

  m_buffered_messages.clear();
}


void Gcs_xcom_communication::cleanup_buffered_messages()
{
  std::vector<Gcs_message *>::iterator buffer_msg_it;

  for (buffer_msg_it= m_buffered_messages.begin();
       buffer_msg_it != m_buffered_messages.end();
       buffer_msg_it++)
  {
    delete *buffer_msg_it;
  }

  m_buffered_messages.clear();
}


size_t Gcs_xcom_communication::number_buffered_messages()
{
  return m_buffered_messages.size();
}
