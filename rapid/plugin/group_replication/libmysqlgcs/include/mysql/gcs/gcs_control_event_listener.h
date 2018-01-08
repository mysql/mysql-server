/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GCS_CONTROL_EVENT_LISTENER_INCLUDED
#define GCS_CONTROL_EVENT_LISTENER_INCLUDED

#include <utility>
#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_message.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"

/**
  Alias for the Data exchanged and delivered from all nodes.

  It shall contain any entry from every member that handed out its data
  for a joining node.
*/
typedef std::vector<std::pair<Gcs_member_identifier *, Gcs_message_data *> >
Exchanged_data;


/**
  @class Gcs_control_event_listener

  This interface is implemented by those who wish to receive Control Interface
  notifications. Currently, it informs about View Changes, delivering the
  underlying installed view.

  For a working example, please refer to the documentation in
  Gcs_communication_interface.
*/
class Gcs_control_event_listener
{
public:
  /**
    This method is called when the view is ready to be installed.

    The contents of Exchanged_data will be released by MySQL GCS
    after this handler finishes. Therefore the application MUST
    copy the contents of exchanged_data if it needs it at a later
    stage.

    @param[in] new_view       a reference to the new view.
    @param[in] exchanged_data the data handed out by other members.
  */

  virtual void on_view_changed(const Gcs_view &new_view,
                               const Exchanged_data &exchanged_data) const= 0;


  /**
    This method is called when the Data Exchange is about to happen in order
    for the client to provide which data it wants to exchange with the group.

    @return a reference to the exchangeable data. This is a pointer that must
            be deallocated by the caller, so please provide always a copy of
            the data to exchange.
  */

  virtual Gcs_message_data* get_exchangeable_data() const= 0;

  /**
    This member function is called when the set of suspicions
    has changed in the underlaying communication infrastructure.

    @param[in] members Contains the list of all members that are in the
                       current view.
    @param[in] unreachable Contains the list of members that are
                           unreachable in the current view, i.e., a subset
                           of @c members.
   */
  virtual void on_suspicions(
          const std::vector<Gcs_member_identifier>& members,
          const std::vector<Gcs_member_identifier>& unreachable) const= 0;

  virtual ~Gcs_control_event_listener() {}
};

#endif // GCS_CONTROL_EVENT_LISTENER_INCLUDED
