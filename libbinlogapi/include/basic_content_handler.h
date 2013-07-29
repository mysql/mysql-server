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
#ifndef BASIC_CONTENT_HANDLER_INCLUDED
#define	BASIC_CONTENT_HANDLER_INCLUDED

#include "binlog_event.h"

namespace mysql {

class Injection_queue : public std::list<mysql::Binary_log_event * >
{
public:
    Injection_queue() : std::list<mysql::Binary_log_event * >() {}
    ~Injection_queue() {}
};

/**
 * A content handler accepts an event and returns the same event,
 * a new one or 0 (the event was consumed by the content handler).
 * The default behaviour is to return the event unaffected.
 * The generic event handler is used for events which aren't routed to
 * a dedicated member function, user defined events being the most
 * common case.
 */

class Content_handler {
public:
  Content_handler();
  Content_handler(const mysql::Content_handler& orig);
  virtual ~Content_handler();

  virtual mysql::Binary_log_event *process_event(mysql::Query_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Row_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Table_map_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Xid *ev);
  virtual mysql::Binary_log_event *process_event(mysql::User_var_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Incident_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Rotate_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Int_var_event *ev);
  virtual mysql::Binary_log_event *process_event(mysql::Format_event *ev);

  /**
    Process any event which hasn't been registered yet.
   */
  virtual mysql::Binary_log_event *process_event(mysql::Binary_log_event *ev);

protected:
  /**
   * The Injection queue is emptied before any new event is pulled from
   * the Binary_log_driver. Injected events will pass through all content
   * handlers. The Injection_queue is a derived std::list.
   */
  Injection_queue *get_injection_queue();

private:
  Injection_queue *m_reinject_queue;
  void set_injection_queue(Injection_queue *injection_queue);
  mysql::Binary_log_event *internal_process_event(mysql::Binary_log_event *ev);

  friend class Binary_log;
};

} // end namespace
#endif	/* BASIC_CONTENT_HANDLER_INCLUDED */
