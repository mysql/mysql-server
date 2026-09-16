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

#include "sql/server_component/mysql_command_session_state_imp.h"

#include <mysql/components/minimal_chassis.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "lex_string.h"
#include "sql/server_component/mysql_command_services_imp.h"
#include "sql_common.h"

namespace {
struct Mysql_command_session_state_iterator {
  std::vector<std::string> items;
  size_t current_item = 0;
};
}  // namespace

DEFINE_METHOD(mysql_command_session_state_status,
              mysql_command_session_state_imp::init,
              (MYSQL_H mysql_h, mysql_command_session_state_type type,
               MYSQL_COMMAND_SESSION_STATE_ITERATOR_H *iterator)) {
  try {
    if (iterator == nullptr) return MYSQL_COMMAND_SESSION_STATE_INVALID;
    *iterator = nullptr;

    auto *mysql_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
    if (mysql_handle == nullptr || mysql_handle->mysql == nullptr ||
        type < MYSQL_COMMAND_SESSION_TRACK_BEGIN ||
        type > MYSQL_COMMAND_SESSION_TRACK_END) {
      return MYSQL_COMMAND_SESSION_STATE_INVALID;
    }

    auto session_state_iterator =
        std::make_unique<Mysql_command_session_state_iterator>();

    // Read the decoded session-tracker cache populated on the MYSQL handle by
    // command-service execution
    auto *info = &MYSQL_EXTENSION_PTR(mysql_handle->mysql)->state_change;
    const auto session_state_type = static_cast<enum_session_state_type>(type);

    // Copy the entries into the iterator
    for (LIST *node = info->info_list[session_state_type].head_node;
         node != nullptr; node = list_rest(node)) {
      auto *item = static_cast<LEX_STRING *>(node->data);

      if (item == nullptr || (item->str == nullptr && item->length > 0)) {
        return MYSQL_COMMAND_SESSION_STATE_ERROR;
      }

      session_state_iterator->items.emplace_back(
          item->str == nullptr ? "" : item->str, item->length);
    }

    if (session_state_iterator->items.empty()) {
      return MYSQL_COMMAND_SESSION_STATE_END;
    }

    *iterator = reinterpret_cast<MYSQL_COMMAND_SESSION_STATE_ITERATOR_H>(
        session_state_iterator.release());

    return MYSQL_COMMAND_SESSION_STATE_OK;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return MYSQL_COMMAND_SESSION_STATE_ERROR;
  }
}

DEFINE_METHOD(mysql_command_session_state_status,
              mysql_command_session_state_imp::get_next,
              (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator, char *buffer,
               size_t buffer_length, size_t *length)) {
  try {
    if (length == nullptr) return MYSQL_COMMAND_SESSION_STATE_INVALID;
    *length = 0;

    auto *session_state_iterator =
        reinterpret_cast<Mysql_command_session_state_iterator *>(iterator);

    if (session_state_iterator == nullptr) {
      return MYSQL_COMMAND_SESSION_STATE_INVALID;
    }

    if (session_state_iterator->current_item >=
        session_state_iterator->items.size()) {
      return MYSQL_COMMAND_SESSION_STATE_END;
    }

    const std::string &item =
        session_state_iterator->items[session_state_iterator->current_item];
    *length = item.length();

    if (buffer == nullptr || buffer_length < item.length()) {
      return MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL;
    }

    if (!item.empty()) memcpy(buffer, item.data(), item.length());

    ++session_state_iterator->current_item;

    return MYSQL_COMMAND_SESSION_STATE_OK;
  } catch (...) {
    mysql_components_handle_std_exception(__func__);
    return MYSQL_COMMAND_SESSION_STATE_ERROR;
  }
}

DEFINE_METHOD(void, mysql_command_session_state_imp::deinit,
              (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator)) {
  auto *session_state_iterator =
      reinterpret_cast<Mysql_command_session_state_iterator *>(iterator);
  delete session_state_iterator;
}
