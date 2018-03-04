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

#include "plugin/group_replication/include/handlers/event_cataloger.h"

Event_cataloger::Event_cataloger() {}

int
Event_cataloger::initialize()
{
  return 0;
}

int
Event_cataloger::terminate()
{
  return 0;
}

int
Event_cataloger::handle_event(Pipeline_event *pevent, Continuation *cont)
{
  Log_event_type event_type= pevent->get_event_type();

  if (event_type == binary_log::TRANSACTION_CONTEXT_EVENT)
  {
    pevent->mark_event(TRANSACTION_BEGIN);
  }
  else if (pevent->get_event_context() != SINGLE_VIEW_EVENT)
  {
    pevent->mark_event(UNMARKED_EVENT);
  }

  //Check if the current transaction was discarded
  if (cont->is_transaction_discarded())
  {
    if ((pevent->get_event_context() == TRANSACTION_BEGIN)||
        (pevent->get_event_context() == SINGLE_VIEW_EVENT))
    {
      //a new transaction begins or we are handling a view change
      cont->set_transation_discarded(false);
    }
    else
    {
      //The event belongs to a discarded transaction
      cont->signal(0, true);
      return 0;
    }
  }

  next(pevent, cont);
  return 0;
}

int Event_cataloger::handle_action(Pipeline_action *action){
  return(next(action));
}

bool Event_cataloger::is_unique()
{
  return true;
}

int Event_cataloger::get_role()
{
  return EVENT_CATALOGER;
}

