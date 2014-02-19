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
#include <sql_class.h>
#include "gcs_message.h"
#include "gcs_protocol.h"
#include "gcs_corosync.h" // todo: describe needs
#include <set>

using std::set;

static void change_node_status(string* uuid, GCS::Member_recovery_status status,
                               bool is_local)
{
  // Nodes that left are not on view, so there is nothing to update.
  if (status != GCS::MEMBER_OFFLINE &&
      cluster_stats.set_node_status(uuid, status))
    log_message(MY_ERROR_LEVEL, "Error updating node '%s' to status '%d'",
                uuid->c_str(), status);

  if (is_local)
    gcs_instance->get_client_info().set_recovery_status(status);
}

static void update_node_status(GCS::Member_set& members,
                               GCS::Member_recovery_status status)
{
  string local_uuid= gcs_instance->get_client_uuid();

  for (std::set<GCS::Member>::iterator it= members.begin();
       it != members.end();
       ++it)
  {
    GCS::Member member = *it;
    bool is_local= !(member.get_uuid().compare(local_uuid));
    change_node_status(&member.get_uuid(), status, is_local);
  }
}

void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate)
{
  string local_uuid= gcs_instance->get_client_uuid();

  if (joined.size() > 0)
    update_node_status(joined, GCS::MEMBER_ONLINE);

  if (left.size() > 0)
    update_node_status(left, GCS::MEMBER_OFFLINE);

  DBUG_ASSERT(view.get_view_id() == 0 || quorate);
  log_view_change(view.get_view_id(), total, left, joined);
}

void handle_message_delivery(GCS::Message *msg, const GCS::View& view)
{
  switch (GCS::get_payload_code(msg))
  {
  case GCS::PAYLOAD_TRANSACTION_EVENT:
    if (applier)
    {
      // andrei todo: raw byte shall be the same in all GCS modules == uchar
      applier->handle((const char*) GCS::get_payload_data(msg),
                      GCS::get_data_len(msg));
    }
    else
    {
      log_message(MY_ERROR_LEVEL, "Message received without a proper applier");
    }
    break;

  default:
    DBUG_ASSERT(0);
  }
};
