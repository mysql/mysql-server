/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef EVENT_CATALOGER_INCLUDE
#define EVENT_CATALOGER_INCLUDE

#include <mysql/group_replication_priv.h>

#include "plugin/group_replication/include/applier.h"

class Event_cataloger : public Event_handler {
 public:
  Event_cataloger();
  int handle_event(Pipeline_event *ev, Continuation *cont) override;
  int handle_action(Pipeline_action *action) override;
  int initialize() override;
  int terminate() override;
  bool is_unique() override;
  int get_role() override;
  /**
    This method handles binary log events by storing them so they can be used on
    next handler.

    @param[in] pevent   the event to be injected
    @param[in] cont     the object used to wait

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int handle_binary_log_event(Pipeline_event *pevent, Continuation *cont);

  /**
    This method handles applier context events by storing them so they can be
    used on next handler.

    @param[in] pevent   the event to be injected
    @param[in] cont     the object used to wait

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
   */
  int handle_applier_event(Pipeline_event *pevent, Continuation *cont);
};

#endif /* EVENT_CATALOGER_INCLUDE */
