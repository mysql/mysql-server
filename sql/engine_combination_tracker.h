// Copyright (c) 2024, Oracle and/or its affiliates.

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.

// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#include <string>
#include <tuple>

class Engine_combination_tracker {
 public:
  /// This function determines whether the current engine is compatible
  /// with previous engine or not.
  ///
  /// @param[in]  engine_name current engine name
  /// @param[in]  database_name current database name
  /// @param[in]  table_name current table name
  /// @param[out] prev_engine_name the incompatible engine name (for logging)
  /// @param[out] prev_database_name the incompatible database (for logging)
  /// @param[out] prev_table_name the incompatible table (for logging)
  ///
  /// @retval true if engine is not compatible
  /// @retval false if engine is compatible

  bool check_engine(std::string engine_name, std::string database_name,
                    std::string table_name, std::string &prev_engine_name,
                    std::string &prev_database_name,
                    std::string &prev_table_name);

  /// This method clear the registered engine in the variable m_known_engine
  void clear_known_engine();

  /// This method get the value of is_warning_already_emitted
  /// @retval true if a warning was already emitted false otherwise
  bool get_warning_already_emitted();

  /// This method sets the value of is_warning_already_emitted
  /// @param value true or false
  void set_warning_already_emitted(bool value);

 private:
  /// The engine data already seen used to check for incompatibilities
  std::tuple<std::string, std::string, std::string> m_known_engine;
  /// If a warning was already emitted for this transactions
  bool is_warning_already_emitted = false;
};