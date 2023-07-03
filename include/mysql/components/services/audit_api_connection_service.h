/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef AUDIT_API_CONNECTION_H
#define AUDIT_API_CONNECTION_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/plugin_audit_connection_types.h>

/**
  @ingroup group_components_services_inventory

  A service to generate Audit API events of the connection class
  (MYSQL_AUDIT_CONNECTION_CLASS).

  The emit method generates the event in the synchronous way, causing
  all subscribers to receive it.

  @sa @ref mysql_audit_api_connection_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_audit_api_connection)

/**
  Method that emits event of the MYSQL_AUDIT_CONNECTION_CLASS class
  and the specified type.

  @sa mysql_event_connection_subclass_t

  @param thd  Session THD that generates connection event.
  @param type Connection event type.

  @return Plugin that receives Audit API event can return event processing
          value. The code that generates the event can take custom action
          based on the returned value. 0 value is returned if no action is
          required on the event generation side.
`*/
DECLARE_METHOD(int, emit, (void *thd, mysql_event_connection_subclass_t type));

END_SERVICE_DEFINITION(mysql_audit_api_connection)

/**
  @ingroup group_components_services_inventory

  A service to generate Audit API events of the connection class
  (MYSQL_AUDIT_CONNECTION_CLASS) with the explicitly specified error code
  value.

  This service should ONLY be used if we cannot set the THD's Statement
  Diagnostic Area result code, which is passed along the audit notification
  chain. The preferred way of generating the event is to rely on THD's internal
  data as much as possible (mysql_audit_api_connection service)

  The emit method generates the event in the synchronous way, causing
  all subscribers to receive it.

  @sa @ref mysql_audit_api_connection_imp
*/
BEGIN_SERVICE_DEFINITION(mysql_audit_api_connection_with_error)

/**
  Method that emits event of the MYSQL_AUDIT_CONNECTION_CLASS class
  and the specified type with the explicitly specified error code value.

  @sa mysql_event_connection_subclass_t

  @param thd     Session THD that generates connection event.
  @param type    Connection event type.
  @param errcode Error code that replaces Statement Diagnostic Area result
                 value, which is simply bypassed by calling this method.

  @return Plugin that receives Audit API event can return event processing
          value. The code that generates the event can take custom action
          based on the returned value. 0 value is returned if no action is
          required on the event generation side.
`*/
DECLARE_METHOD(int, emit,
               (void *thd, mysql_event_connection_subclass_t type,
                int errcode));

END_SERVICE_DEFINITION(mysql_audit_api_connection_with_error)

#endif /* AUDIT_API_CONNECTION_H */
