/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CONNECTION_DELAY_API_H
#define CONNECTION_DELAY_API_H

/* Structures/Functions requried for information schema table */

extern struct st_mysql_information_schema connection_control_failed_attempts_view;
int connection_control_failed_attempts_view_init(void *ptr);

namespace connection_control
{

  /* constants/variables defined in connection_delay.cc */

  extern int64 DEFAULT_THRESHOLD;
  extern int64 MIN_THRESHOLD;
  extern int64 DISABLE_THRESHOLD;
  extern int64 MAX_THRESHOLD;

  extern int64 DEFAULT_MAX_DELAY;
  extern int64 DEFAULT_MIN_DELAY;
  extern int64 MIN_DELAY;
  extern int64 MAX_DELAY;

  /** Functions being used by connection_control.cc */

  class Connection_event_coordinator_services;
  class Error_handler;

  bool init_connection_delay_event(Connection_event_coordinator_services *coordinator,
                                   Error_handler *error_handler);
  void deinit_connection_delay_event();

}
#endif // !CONNECTION_DELAY_API_H
