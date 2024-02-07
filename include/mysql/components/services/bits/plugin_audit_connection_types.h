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

Without limiting anything contained in the foregoing, this file,
which is part of C Driver for MySQL (Connector/C), is also subject to the
Universal FOSS Exception, version 1.0, a copy of which can be found at
http://oss.oracle.com/licenses/universal-foss-exception.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_AUDIT_CONNECTION_TYPES_H
#define PLUGIN_AUDIT_CONNECTION_TYPES_H

/**
  @enum mysql_event_connection_subclass_t

  Events for MYSQL_AUDIT_CONNECTION_CLASS event class.
*/
typedef enum {
  /** occurs after authentication phase is completed. */
  MYSQL_AUDIT_CONNECTION_CONNECT = 1 << 0,
  /** occurs after connection is terminated. */
  MYSQL_AUDIT_CONNECTION_DISCONNECT = 1 << 1,
  /** occurs after COM_CHANGE_USER RPC is completed. */
  MYSQL_AUDIT_CONNECTION_CHANGE_USER = 1 << 2,
  /** occurs before authentication. */
  MYSQL_AUDIT_CONNECTION_PRE_AUTHENTICATE = 1 << 3
} mysql_event_connection_subclass_t;

#endif /* PLUGIN_AUDIT_CONNECTION_TYPES_H */
