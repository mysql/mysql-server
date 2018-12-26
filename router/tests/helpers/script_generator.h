/*
 Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef TESTS_COMPONENT_SCRIPT_MANAGER_H_
#define TESTS_COMPONENT_SCRIPT_MANAGER_H_

#include "mysql/harness/filesystem.h"

class ScriptGenerator {
  mysql_harness::Path bin_path_;
  mysql_harness::Path tmp_path_;

 public:
  ScriptGenerator(const mysql_harness::Path &bin_path,
                  const std::string &tmp_directory)
      : bin_path_(bin_path), tmp_path_{tmp_directory} {}

  std::string get_reader_incorrect_master_key_script() const;
  std::string get_reader_script() const;
  std::string get_writer_script() const;
  std::string get_writer_exec() const;
  std::string get_fake_reader_script() const;
  std::string get_fake_writer_script() const;
};  // end of script manager

#endif /* TESTS_COMPONENT_SCRIPT_MANAGER_H_ */
