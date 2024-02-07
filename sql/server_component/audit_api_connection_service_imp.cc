/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "audit_api_connection_service_imp.h"

#include "sql/current_thd.h"
#include "sql/sql_audit.h"

DEFINE_METHOD(int, mysql_audit_api_connection_imp::emit,
              (void *thd, mysql_event_connection_subclass_t type)) {
  THD *t = static_cast<THD *>(thd);
  switch (type) {
    case MYSQL_AUDIT_CONNECTION_CONNECT:
      mysql_audit_enable_auditing(t);
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_CONNECT));
      break;
    case MYSQL_AUDIT_CONNECTION_DISCONNECT:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_DISCONNECT));
      break;
    case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_CHANGE_USER));
      break;
    case MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE));
      break;
    default:
      break;
  }

  return 0;
}

DEFINE_METHOD(int, mysql_audit_api_connection_with_error_imp::emit,
              (void *thd, mysql_event_connection_subclass_t type,
               int errcode)) {
  THD *t = static_cast<THD *>(thd);
  switch (type) {
    case MYSQL_AUDIT_CONNECTION_CONNECT:
      mysql_audit_enable_auditing(t);
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_CONNECT), errcode);
      break;
    case MYSQL_AUDIT_CONNECTION_DISCONNECT:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_DISCONNECT), errcode);
      break;
    case MYSQL_AUDIT_CONNECTION_CHANGE_USER:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_CHANGE_USER), errcode);
      break;
    case MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE:
      return mysql_event_tracking_connection_notify(
          t, AUDIT_EVENT(EVENT_TRACKING_CONNECTION_PRE_AUTHENTICATE), errcode);
      break;
    default:
      break;
  }

  return 0;
}
