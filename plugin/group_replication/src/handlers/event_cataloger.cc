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

#include "plugin/group_replication/include/handlers/event_cataloger.h"

Event_cataloger::Event_cataloger() = default;

int Event_cataloger::initialize() { return 0; }

int Event_cataloger::terminate() { return 0; }

int Event_cataloger::handle_event(Pipeline_event *pevent, Continuation *cont) {
  DBUG_TRACE;
  Pipeline_event::Pipeline_event_type event_type =
      pevent->get_pipeline_event_type();
  switch (event_type) {
    case Pipeline_event::Pipeline_event_type::PEVENT_DATA_PACKET_TYPE_E:
      return handle_binary_log_event(pevent, cont);
    case Pipeline_event::Pipeline_event_type::PEVENT_BINARY_LOG_EVENT_TYPE_E:
      return handle_binary_log_event(pevent, cont);
    case Pipeline_event::Pipeline_event_type::PEVENT_APPLIER_ONLY_EVENT_E:
      return handle_applier_event(pevent, cont);
    default:
      next(pevent, cont);
      return 0;
  }
}

int Event_cataloger::handle_binary_log_event(Pipeline_event *pevent,
                                             Continuation *cont) {
  mysql::binlog::event::Log_event_type event_type = pevent->get_event_type();

  if (event_type == mysql::binlog::event::TRANSACTION_CONTEXT_EVENT) {
    pevent->mark_event(TRANSACTION_BEGIN);
  } else if (pevent->get_event_context() != SINGLE_VIEW_EVENT) {
    pevent->mark_event(UNMARKED_EVENT);
  }

  // Check if the current transaction was discarded
  if (cont->is_transaction_discarded()) {
    if ((pevent->get_event_context() == TRANSACTION_BEGIN) ||
        (pevent->get_event_context() == SINGLE_VIEW_EVENT)) {
      // a new transaction begins or we are handling a view change
      cont->set_transation_discarded(false);
    } else {
      // The event belongs to a discarded transaction
      cont->signal(0, true);
      return 0;
    }
  }
  next(pevent, cont);
  return 0;
}

int Event_cataloger::handle_applier_event(Pipeline_event *event,
                                          Continuation *cont) {
  return next(event, cont);
}

int Event_cataloger::handle_action(Pipeline_action *action) {
  return (next(action));
}

bool Event_cataloger::is_unique() { return true; }

int Event_cataloger::get_role() { return EVENT_CATALOGER; }
