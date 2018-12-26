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

#include <sys/stat.h>
#include <fstream>
#include <string>

#include "script_generator.h"

using mysql_harness::Path;

std::string ScriptGenerator::get_reader_incorrect_master_key_script() const {
#ifdef _WIN32
  Path script(tmp_path_.join("reader_script.bat").str());
#else
  Path script(tmp_path_.join("reader_script.sh").str());
#endif
  {
    std::ofstream file(script.str());
#ifdef _WIN32
    file << "@echo off" << std::endl;
    file << "echo master_key_123";
#else
    file << "#!/bin/bash" << std::endl;
    file << "echo master_key_123" << std::endl;
#endif
  }
#ifndef _WIN32
  chmod(script.c_str(), 0700);
#endif
  return script.c_str();
}

std::string ScriptGenerator::get_reader_script() const {
#ifdef _WIN32
  Path script(tmp_path_.join("reader_script.bat").str());
#else
  Path script(tmp_path_.join("reader_script.sh").str());
#endif
  {
    std::ofstream file(script.str());
#ifdef _WIN32
    file << "@echo off" << std::endl;
    std::string file_path = tmp_path_.join("master_key").str();
    file << "type nul >> " << file_path << std::endl;
    for (char &c : file_path)
      if (c == '/') c = '\\';
    file << "type " << file_path << std::endl;
#else
    file << "#!/bin/bash" << std::endl;
    file << "touch " << tmp_path_.join("master_key").str() << std::endl;
    file << "cat " << tmp_path_.join("master_key").str() << std::endl;
#endif
  }
#ifndef _WIN32
  chmod(script.c_str(), 0700);
#endif
  return script.c_str();
}

std::string ScriptGenerator::get_writer_script() const {
  Path writer_path = bin_path_.join(get_writer_exec());
  return writer_path.str();
}

std::string ScriptGenerator::get_writer_exec() const {
  std::string master_key_path = tmp_path_.join("master_key").str();
  // add master_key location to environment: MASTER_KEY_PATH
  int err_code;
#ifdef _WIN32
  err_code = _putenv_s("MASTER_KEY_PATH", master_key_path.c_str());
#else
  err_code = ::setenv("MASTER_KEY_PATH", master_key_path.c_str(), 1);
#endif
  if (err_code) throw std::runtime_error("Failed to add MASTER_KEY_PATH");

#ifdef _WIN32
  return "master_key_test_writer.exe";
#else
  return "master_key_test_writer";
#endif
}

std::string ScriptGenerator::get_fake_reader_script() const {
  return "fake_reader_script";
}

std::string ScriptGenerator::get_fake_writer_script() const {
  return "fake_writer_script";
}
