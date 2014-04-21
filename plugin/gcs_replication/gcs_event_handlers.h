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

#ifndef GCS_EVENT_HANDLERS_INCLUDE
#define GCS_EVENT_HANDLERS_INCLUDE

#include <gcs_protocol.h>
#include <gcs_protocol_factory.h>
#include "gcs_member_info.h"

/*
  The function is called at View change and receives the view and three set of
  members as arguments:

  @param[in]  view    the view being installed
  @param[in]  total   the members in the view being installed
  @param[in]  left    the members that left the view
  @param[in]  joined  the members that joined the view


  @note Using that info this function implements a prototype of Node manager
  that checks quorate condition and terminates this instance membership when
  it does not hold.

  @note This definition is only for testing purposes for now.
*/
void handle_view_change(GCS::View& view, GCS::Member_set& total,
                        GCS::Member_set& left, GCS::Member_set& joined,
                        bool quorate);

/*
  The function is invoked whenever a message is delivered from a group.

  @param[in] msg     pointer to Message object
  @param[in] view    pointer to the View in which delivery is happening
*/
void handle_message_delivery(GCS::Message *msg, const GCS::View& view);

/**
  Takes all the joining members handling status changes and recovery related
  tasks.

  @param view         the received view
  @param joined       the joined members
  @param total        the set of all group members
  @param is_joining   is the local node joining
 */
void handle_joining_nodes(GCS::View& view,
                          GCS::Member_set& joined,
                          GCS::Member_set& total,
                          bool is_joining);

/**
  Handles state change and recovery related tasks for all the leaving members

  @param left          the leaving members
  @param total        the set of all group members
  @param is_joining    is the local node joining
*/
void handle_leaving_nodes(GCS::Member_set& left,
                          GCS::Member_set& total,
                          bool is_joining);
/**
  Handle a transaction based message received through gcs

  @param msg  the received message
 */
void handle_transactional_message(GCS::Message *msg);

/**
  Handle a certifier based message received through gcs

  @param msg  the received message
 */
void handle_certifier_message(GCS::Message *msg);

/**
  Handle a recovery based message received through gcs

  @param msg  the received message
 */
void handle_recovery_message(GCS::Message *msg);


#endif /* GCS_EVENT_HANDLERS_INCLUDE */
