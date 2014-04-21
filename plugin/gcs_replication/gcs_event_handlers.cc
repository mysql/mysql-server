/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "gcs_event_handlers.h"
#include "gcs_plugin.h"
#include "gcs_certifier.h"
#include "gcs_recovery.h"
#include <sql_class.h>
#include "gcs_message.h"
#include "gcs_protocol.h"
#include "gcs_payload.h"
#include "gcs_recovery_message.h"
#include <set>
#include <string>

using GCS::View;
using GCS::Member_set;
using GCS::Member;
using GCS::Member_recovery_status;

using std::set;

/**
  Updates all node with the given status.
  If the node is a local node then the status is also changed on the protocol
  object so the correct state is transmited on view changes.

  @note if the given status is "GCS::MEMBER_OFFLINE" then the state is only
  update in the protocol for the local node, as all the other offline members
  simply disapear from the group.

  @param uuid        the member uuid
  @param status      the status to which the node should change
  @param is_local    is the node local or not
*/
static void change_node_status(string* uuid, GCS::Member_recovery_status status,
                               bool is_local)
{
  // Nodes that left are not on view, so there is nothing to update.
  if (status != GCS::MEMBER_OFFLINE &&
      cluster_stats.set_node_status(uuid, status))
    log_message(MY_ERROR_LEVEL, "Error updating node '%s' to status '%d'",
                uuid->c_str(), status);

  if (is_local)
    gcs_module->get_client_info().set_recovery_status(status);
}

/**
  Updates all nodes in the given set to the given status if their base status
  is equal to the given condition one.

  @note if no condition exists to update, pass GCS::MEMBER_END as the condition.

  @param members           the members to update
  @param status            the status to which the nodes should change
  @param condition_status  the condition status to be tested
*/
static void update_node_status(GCS::Member_set& members,
                               GCS::Member_recovery_status status,
                               GCS::Member_recovery_status condition_status)
{
  string local_uuid= gcs_module->get_client_uuid();

  for (std::set<GCS::Member>::iterator it= members.begin();
       it != members.end();
       ++it)
  {
    GCS::Member member = *it;
    bool is_local= !(member.get_uuid().compare(local_uuid));
    if(condition_status == GCS::MEMBER_END ||
       member.get_recovery_status() == condition_status)
    {
      change_node_status(&member.get_uuid(), status, is_local);
    }
  }
}

void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate)
{
  string node_uuid= gcs_module->get_client_uuid();
  Member_recovery_status node_status= cluster_stats.get_node_status(&node_uuid);
  //if we are offline at this point it means the local node is joining.
  bool is_joining= (node_status == GCS::MEMBER_OFFLINE);

  //Handle joining members and calculates if we are joining.
  handle_joining_nodes(view, joined, total, is_joining);
  //Update any recovery running process and handle state changes
  handle_leaving_nodes(left, total, is_joining);

  DBUG_ASSERT(view.get_view_id() == 0 || quorate);
  log_view_change(view.get_view_id(), total, left, joined);

  Certifier_interface *certifier= applier_module->get_certification_handler()->get_certifier();
  certifier->handle_view_change();
}

void handle_message_delivery(GCS::Message *msg, const GCS::View& view)
{
  switch (GCS::get_payload_code(msg))
  {
  case GCS::PAYLOAD_TRANSACTION_EVENT:
    handle_transactional_message(msg);
    break;

  case GCS::PAYLOAD_CERTIFICATION_EVENT:
    handle_certifier_message(msg);
    break;

  case GCS::PAYLOAD_RECOVERY_EVENT:
    handle_recovery_message(msg);
    break;

  default:
    DBUG_ASSERT(0);
  }
}

void handle_joining_nodes(GCS::View& view,
                         GCS::Member_set& joined,
                         GCS::Member_set& total,
                         bool is_joining)
{
  //nothing to do here
  if(joined.size() == 0)
  {
    return;
  }
  /**
   On the joining list there can be 2 types of members: online/recovering nodes
   coming from old views where this node was not present and new joining nodes
   that still have their status as offline.

   As so, nodes that are offline, their state is changed to member_in_recovery.
  */
  update_node_status(joined, GCS::MEMBER_IN_RECOVERY, GCS::MEMBER_OFFLINE);

  /*
   If we are joining, two scenarios exist
   1) We are alone so we declare ourselves online
   2) We are in a cluster and recovery must happen
  */
  if (is_joining)
  {
    if ((int)total.size() == 1)
    {
      log_message(MY_INFORMATION_LEVEL,
                  "[Recovery:] Only one node alive. "
                  "Declaring the node online.");
      change_node_status(&gcs_module->get_client_uuid(), GCS::MEMBER_ONLINE, true);
    }
    else //start recovery
    {
      log_message(MY_INFORMATION_LEVEL,
                  "[Recovery:] Starting recovery with view_id %llu",
                  view.get_view_id());
      /*
       During the view change, a suspension packet is sent to the applier module
       so all posterior transactions inbound are not applied, but queued, until
       the node finishes recovery.
      */
      applier_module->add_suspension_packet();

      /*
       Launch the recovery thread so we can receive missing data and the
       certification information needed to apply the transactions queued after
       this view change.

       Recovery receives a view id, as a means to identify logically on joiners
       and donors alike where this view change happened in the data. With that
       info we can then ask for the donor to give the node all the data until
       this point in the data, and the certification information for all the data
       that comes next.
      */
      recovery_module->start_recovery(view.get_group_name(), view.get_view_id(), total);
    }
  }
  else
  {
    log_message(MY_INFORMATION_LEVEL,
                "[Recovery:] Marking view change with view_id %llu",
                view.get_view_id());
    /**
     If not a joining member, all nodes should record on their own binlogs a
     marking event that identifies the frontier between the data the joining
     node was to receive and the data it should queue.
     The joining node can then wait for this event to know it was all the needed
     data.

     This packet will also pass in the certification process at this exact
     frontier giving us the opportunity to gather the necessary certification
     information to certify the transactions that will come after this view
     change. If selected as a donor, this info will also be sent to the joiner.
    */
    applier_module->add_view_change_packet(view.get_view_id());
  }
}

void handle_leaving_nodes(GCS::Member_set& left, GCS::Member_set& total,
                          bool joining)
{
  string node_uuid= gcs_module->get_client_uuid();
  Member_recovery_status node_status= cluster_stats.get_node_status(&node_uuid);

  //if the node is joining or in recovery, no need to update the process
  if(!joining && node_status == GCS::MEMBER_IN_RECOVERY)
  {
    /*
     This method has 2 purposes:
     If a donor leaves, recovery needs to switch donor
     If this node leaves, recovery needs to shutdown.
    */
    recovery_module->update_recovery_process(left, total);
  }

  if (left.size() > 0)
  {
    update_node_status(left, GCS::MEMBER_OFFLINE, GCS::MEMBER_END /* No condition*/);
  }
}


void handle_transactional_message(GCS::Message *msg)
{
  if (applier_module)
  {
    // andrei todo: raw byte shall be the same in all GCS modules == uchar
    applier_module->handle((const char*) GCS::get_payload_data(msg),
                      GCS::get_data_len(msg));
  }
  else
  {
    log_message(MY_ERROR_LEVEL, "Message received without a proper applier");
  }
}

void handle_certifier_message(GCS::Message *msg)
{
  if (applier_module == NULL)
  {
    log_message(MY_ERROR_LEVEL, "Message received without a proper applier");
    return;
  }

  Certifier_interface *certifier= applier_module->get_certification_handler()->get_certifier();
  if (certifier->handle_certifier_data((const char*) GCS::get_payload_data(msg),
                                       GCS::get_data_len(msg)))
  {
      log_message(MY_ERROR_LEVEL, "Error processing payload information event");
  }
}

void handle_recovery_message(GCS::Message *msg)
{
  size_t data_len= GCS::get_data_len(msg);
  const uchar* data= GCS::get_payload_data(msg);
  Recovery_message *recovery_message= new Recovery_message(data, data_len);

  string *node_uuid= recovery_message->get_node_uuid();
  bool is_local= !node_uuid->compare(gcs_module->get_client_uuid());

  // The node is declare as online upon receiving this message
  change_node_status(node_uuid, GCS::MEMBER_ONLINE, is_local);

  if(is_local)
  {
    log_message(MY_INFORMATION_LEVEL,
                "[Recovery:] This node was declared online");
  }
  else
  {
    log_message(MY_INFORMATION_LEVEL,
                "[Recovery:] Node %s was declared online",
                node_uuid->c_str());
  }
}

