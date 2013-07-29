/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#include "binlog_event.h"
#include "basic_transaction_parser.h"
#include "protocol.h"
#include "value.h"
#include "field_iterator.h"
#include <iostream>

namespace mysql {

mysql::Binary_log_event *Basic_transaction_parser::
process_event(mysql::Query_event *qev)
{
  if (qev->query == "BEGIN")
  {
    m_transaction_state= STARTING;
  }
  else if (qev->query == "COMMIT")
  {
    m_transaction_state= COMMITTING;
  }

  return process_transaction_state(qev);
}

mysql::Binary_log_event *Basic_transaction_parser::
process_event(mysql::Xid *ev)
{
  m_transaction_state= COMMITTING;
  return process_transaction_state(ev);
}

mysql::Binary_log_event *Basic_transaction_parser::
process_event(mysql::Table_map_event *ev)
{
  if(m_transaction_state ==IN_PROGRESS)
  {
    m_event_stack.push_back(ev);
    return 0;
  }
  return ev;
}

mysql::Binary_log_event *Basic_transaction_parser::
process_event(mysql::Row_event *ev)
{
  if(m_transaction_state ==IN_PROGRESS)
  {
    m_event_stack.push_back(ev);
    return 0;
  }
  return ev;
}

mysql::Binary_log_event *Basic_transaction_parser::
process_transaction_state(mysql::Binary_log_event *incomming_event)
{
  switch(m_transaction_state)
  {
    case STARTING:
    {
      m_transaction_state= IN_PROGRESS;
      m_start_time= incomming_event->header()->timestamp;
      delete incomming_event; // drop the begin event
      return 0;
    }
    case COMMITTING:
    {
      delete incomming_event; // drop the commit event

      /**
       * Propagate the start time for the transaction to the newly created
       * event.
       */
      mysql::Transaction_log_event *trans=
      mysql::create_transaction_log_event();
      trans->header()->timestamp= m_start_time;

      while( m_event_stack.size() > 0)
      {
        mysql::Binary_log_event *event= m_event_stack.front();
        m_event_stack.pop_front();
        switch(event->get_event_type())
        {
          case mysql::TABLE_MAP_EVENT:
          {
            /*
             Index the table name with a table id to ease lookup later.
            */
            mysql::Table_map_event *tm=
            static_cast<mysql::Table_map_event *>(event);
            trans->m_table_map.insert(mysql::
                                      Event_index_element(tm->table_id,tm));
            trans->m_events.push_back(event);
          }
          break;
          case mysql::WRITE_ROWS_EVENT:
          case mysql::WRITE_ROWS_EVENT_V1:
          case mysql::DELETE_ROWS_EVENT:
          case mysql::DELETE_ROWS_EVENT_V1:
          case mysql::UPDATE_ROWS_EVENT:
          case mysql::UPDATE_ROWS_EVENT_V1:
          {
            trans->m_events.push_back(event);
             /*
             * Propagate last known next position
             */
            trans->header()->next_position= event->header()->next_position;
          }
          break;
          default:
            delete event;
         }
      } // end while
      m_transaction_state= NOT_IN_PROGRESS;
      return(trans);
    }
    case NOT_IN_PROGRESS:
    default:
      return incomming_event;
  }

}

Transaction_log_event *create_transaction_log_event(void)
{
    Transaction_log_event *trans= new Transaction_log_event();
    trans->header()->type_code= mysql::USER_DEFINED;
    return trans;
};

Transaction_log_event::~Transaction_log_event()
{
  Int_to_Event_map::iterator it;
  for(it = m_table_map.begin(); it != m_table_map.end();)
  {
    /* No need to delete the event here; it happens in the next iteration */
    m_table_map.erase(it++);
  }

  while (m_events.size() > 0)
  {
    Binary_log_event *event= m_events.back();
    m_events.pop_back();
    delete(event);
  }

}

} // end namespace
