// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_CSA_SLIDING_WINDOW_COUNTER_H
#define MYSQL_CSA_SLIDING_WINDOW_COUNTER_H

namespace mysql::csa {

/// @brief Calculates statistic value in a time window
struct Sliding_window_counter {
  /// @brief Previous global counter
  unsigned long long m_previous{0};
  /// @brief Counter in the current window
  unsigned long long m_value{0};
  /// @brief Updates the window counter
  /// @param current Global value, registered at current point of time
  void update(unsigned long long current);
  /// @brief Obtains the value in the current time window
  unsigned long long get() const;
};

}  // namespace mysql::csa

#endif  // MYSQL_CSA_SLIDING_WINDOW_COUNTER_H
