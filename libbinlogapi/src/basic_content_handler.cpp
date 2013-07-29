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
#include "basic_content_handler.h"

namespace mysql {

Content_handler::Content_handler () {}
Content_handler::~Content_handler () {}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Query_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Row_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Table_map_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Xid *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::User_var_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Incident_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Rotate_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Int_var_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Format_event *ev)
{
  return ev;
}
mysql::Binary_log_event *Content_handler::
process_event(mysql::Binary_log_event *ev)
{
  return ev;
}

Injection_queue *Content_handler::
get_injection_queue(void)
{
  return m_reinject_queue;
}

void Content_handler::
set_injection_queue(Injection_queue *queue)
{
  m_reinject_queue= queue;
}

mysql::Binary_log_event*
  Content_handler::internal_process_event(mysql::Binary_log_event *ev)
{
 mysql::Binary_log_event *processed_event= 0;
 switch(ev->header ()->type_code) {
 case QUERY_EVENT:
   processed_event= process_event(static_cast<mysql::Query_event*>(ev));
   break;
 case WRITE_ROWS_EVENT:
 case WRITE_ROWS_EVENT_V1:
 case UPDATE_ROWS_EVENT:
 case UPDATE_ROWS_EVENT_V1:
 case DELETE_ROWS_EVENT:
 case DELETE_ROWS_EVENT_V1:
   processed_event= process_event(static_cast<mysql::Row_event*>(ev));
   break;
 case USER_VAR_EVENT:
   processed_event= process_event(static_cast<mysql::User_var_event *>(ev));
   break;
 case ROTATE_EVENT:
   processed_event= process_event(static_cast<mysql::Rotate_event *>(ev));
   break;
 case INCIDENT_EVENT:
   processed_event= process_event(static_cast<mysql::Incident_event *>(ev));
   break;
 case XID_EVENT:
   processed_event= process_event(static_cast<mysql::Xid *>(ev));
   break;
 case TABLE_MAP_EVENT:
   processed_event= process_event(static_cast<mysql::Table_map_event *>(ev));
   break;
 case FORMAT_DESCRIPTION_EVENT:
   processed_event= process_event(static_cast<mysql::Format_event *>(ev));
   break;
 /* TODO ********************************************************************/
 case BEGIN_LOAD_QUERY_EVENT:
   processed_event= process_event(ev);
   break;
 case EXECUTE_LOAD_QUERY_EVENT:
   processed_event= process_event(ev);
   break;
 case INTVAR_EVENT:
   processed_event= process_event(ev);
   break;
 case STOP_EVENT:
   processed_event= process_event(ev);
   break;
 case RAND_EVENT:
   processed_event= process_event(ev);
   break;
 /****************************************************************************/
 default:
   processed_event= process_event(ev);
   break;
 }
 return processed_event;
}

} // end namespace
