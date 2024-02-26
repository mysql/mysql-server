#ifndef PERSISTED_RULE_INCLUDED
#define PERSISTED_RULE_INCLUDED
/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include <memory>
#include <optional>
#include <string>
#include "my_config.h"
#include "mysql/service_rules_table.h"

namespace rts = rules_table_service;

/**
  @file persisted_rule.h

  The facilities for easily manipulating nullable values from a
  rules_table_service::Cursor.
*/

/// A rule as persisted on disk.
class Persisted_rule {
 public:
  /// The rewrite rule's pattern string.
  std::optional<std::string> pattern;

  /// The pattern's current database.
  std::optional<std::string> pattern_db;

  /// The rewrite rule's replacement string.
  std::optional<std::string> replacement;

  /// True if the rule is enabled.
  bool is_enabled;

  /// The plugin's message, write-only.
  std::optional<std::string> message;

  /// The pattern's digest, write-only.
  std::optional<std::string> pattern_digest;

  /// The normalized pattern, write-only.
  std::optional<std::string> normalized_pattern;

  /**
    Constructs a Persisted_rule object that copies all data into the current
    heap. The interface is constructed this way due to on some OS'es
    (e.g. Windows), every shared library has its own heap.
  */
  explicit Persisted_rule(rts::Cursor *c) {
    copy_and_set(&pattern, c, c->pattern_column());
    copy_and_set(&pattern_db, c, c->pattern_database_column());
    copy_and_set(&replacement, c, c->replacement_column());

    const char *is_enabled_c = (c->fetch_string(c->enabled_column()));
    if (is_enabled_c != nullptr && is_enabled_c[0] == 'Y')
      is_enabled = true;
    else
      is_enabled = false;
    rts::free_string(is_enabled_c);
  }

  /// Convenience function, may be called with a const char*.
  void set_message(const std::string &message_arg) {
    message = std::optional<std::string>(message_arg);
  }

  /// Convenience function, may be called with a const char*.
  void set_pattern_digest(const std::string &s) {
    pattern_digest = std::optional<std::string>(s);
  }

  /// Convenience function, may be called with a const char*.
  void set_normalized_pattern(const std::string &s) {
    normalized_pattern = std::optional<std::string>(s);
  }

  /**
    Writes the values in this Persisted_rule to the table at the row pointed
    to by the cursor. Values that don't have a corresponding column in the
    table will be ignored.
  */
  bool write_to(rts::Cursor *c) {
    c->make_writeable();

    set_if_present(c, c->message_column(), message);
    set_if_present(c, c->pattern_digest_column(), pattern_digest);
    set_if_present(c, c->normalized_pattern_column(), normalized_pattern);

    return c->write();
  }

 private:
  /**
    Reads from a Cursor and writes to a property of type std::optional<string>
    after forcing a copy of the string buffer. The function calls a member
    function in Cursor that is located in the server's dynamic library.
  */
  void copy_and_set(std::optional<std::string> *property, rts::Cursor *c,
                    int colno) {
    const char *value = c->fetch_string(colno);
    if (value != nullptr) {
      std::string tmp;
      tmp.assign(value);
      *property = tmp;
    }
    rts::free_string(value);
  }

  /// Writes a string value to the cursor's column if it exists.
  void set_if_present(rts::Cursor *cursor, rts::Cursor::column_id column,
                      std::optional<std::string> value) {
    if (column == rts::Cursor::ILLEGAL_COLUMN_ID) return;
    if (!value.has_value()) {
      cursor->set(column, nullptr, 0);
      return;
    }
    const std::string &s = value.value();
    cursor->set(column, s.c_str(), s.length());
  }
};

#endif  // PERSISTED_RULE_INCLUDED
