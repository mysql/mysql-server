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

#include "sql/engine_combination_tracker.h"
#include <iostream>

bool Engine_combination_tracker::check_engine(std::string engine_name,
                                              std::string database_name,
                                              std::string table_name,
                                              std::string &prev_engine_name,
                                              std::string &prev_database_name,
                                              std::string &prev_table_name) {
  if (std::get<0>(m_known_engine) == "") {
    m_known_engine = std::make_tuple(engine_name, database_name, table_name);
    return false;
  }
  if (std::get<0>(m_known_engine) == engine_name) return false;

  bool myisam_and_merge = ((engine_name == "MyISAM" &&
                            std::get<0>(m_known_engine) == "MRG_MYISAM") ||
                           (engine_name == "MRG_MYISAM" &&
                            std::get<0>(m_known_engine) == "MyISAM"));
  bool innodb_and_blackhole =
      ((engine_name == "InnoDB" &&
        std::get<0>(m_known_engine) == "BLACKHOLE") ||
       (engine_name == "BLACKHOLE" && std::get<0>(m_known_engine) == "InnoDB"));

  if (myisam_and_merge || innodb_and_blackhole) return false;

  prev_engine_name = std::get<0>(m_known_engine);
  prev_database_name = std::get<1>(m_known_engine);
  prev_table_name = std::get<2>(m_known_engine);
  return true;
}

void Engine_combination_tracker::clear_known_engine() {
  m_known_engine = std::make_tuple("", "", "");
}

bool Engine_combination_tracker::get_warning_already_emitted() {
  return is_warning_already_emitted;
}

void Engine_combination_tracker::set_warning_already_emitted(bool value) {
  is_warning_already_emitted = value;
}