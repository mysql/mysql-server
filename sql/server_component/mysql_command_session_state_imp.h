/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_COMMAND_SESSION_STATE_IMP_H
#define MYSQL_COMMAND_SESSION_STATE_IMP_H

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_command_session_state.h>

class mysql_command_session_state_imp {
 public:
  static DEFINE_METHOD(mysql_command_session_state_status, init,
                       (MYSQL_H mysql_h, mysql_command_session_state_type type,
                        MYSQL_COMMAND_SESSION_STATE_ITERATOR_H *iterator));
  static DEFINE_METHOD(mysql_command_session_state_status, get_next,
                       (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator,
                        char *buffer, size_t buffer_length, size_t *length));
  static DEFINE_METHOD(void, deinit,
                       (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator));
};

#endif /* MYSQL_COMMAND_SESSION_STATE_IMP_H */
