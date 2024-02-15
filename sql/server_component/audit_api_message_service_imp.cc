/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "audit_api_message_service_imp.h"

#include "sql/current_thd.h"
#include "sql/sql_audit.h"

#include <memory>

DEFINE_BOOL_METHOD(mysql_audit_api_message_imp::emit,
                   (mysql_event_message_subclass_t type, const char *component,
                    size_t component_length, const char *producer,
                    size_t producer_length, const char *message,
                    size_t message_length,
                    mysql_event_message_key_value_t *key_value_map,
                    size_t key_value_map_length)) {
  std::unique_ptr<mysql_event_tracking_message_key_value_t[]>
      local_key_value_map(key_value_map_length > 0
                              ? new mysql_event_tracking_message_key_value_t
                                    [key_value_map_length]
                              : nullptr);

  mysql_event_tracking_message_key_value_t *local_kv =
      local_key_value_map.get();
  mysql_event_message_key_value_t *kv = key_value_map;

  for (size_t i = 0; i < key_value_map_length; ++i, ++local_kv, ++kv) {
    local_kv->key = {kv->key.str, kv->key.length};
    switch (kv->value_type) {
      case MYSQL_AUDIT_MESSAGE_VALUE_TYPE_STR:
        local_kv->value_type = EVENT_TRACKING_MESSAGE_VALUE_TYPE_STR;
        local_kv->value.str = {kv->value.str.str, kv->value.str.length};
        break;
      case MYSQL_AUDIT_MESSAGE_VALUE_TYPE_NUM:
        local_kv->value_type = EVENT_TRACKING_MESSAGE_VALUE_TYPE_NUM;
        local_kv->value.num = kv->value.num;
        break;
      default:
        assert(false);
        break;
    }
  }

  if (type == MYSQL_AUDIT_MESSAGE_INTERNAL)
    mysql_event_tracking_message_notify(
        current_thd, AUDIT_EVENT(EVENT_TRACKING_MESSAGE_INTERNAL), component,
        component_length, producer, producer_length, message, message_length,
        local_key_value_map.get(), key_value_map_length);
  else
    mysql_event_tracking_message_notify(
        current_thd, AUDIT_EVENT(EVENT_TRACKING_MESSAGE_USER), component,
        component_length, producer, producer_length, message, message_length,
        local_key_value_map.get(), key_value_map_length);

  return false;
}
