/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

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

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include "my_dbug.h"

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::nanoseconds;
using std::chrono::time_point;

void log_prefix(std::ostream &out) {
  time_point<high_resolution_clock> tp1;
  tp1 = high_resolution_clock::now();
  std::chrono::nanoseconds ns =
      duration_cast<nanoseconds>(tp1.time_since_epoch());
  std::thread::id thread_id = std::this_thread::get_id();

  out << "ts=" << ns.count() << ", thread=" << thread_id << ": ";
}

static std::mutex g_mutex;
static std::list<std::string> g_debug_lst;
static const unsigned int MAX_NOTES = 20000;

void trace(const std::string &note) {
  std::lock_guard<std::mutex> guard(g_mutex);

  g_debug_lst.push_back(note);

  if (g_debug_lst.size() > MAX_NOTES) {
    g_debug_lst.pop_front();
  }
}

void fn_print_string(const std::string &m) { std::cout << m << std::endl; }

void dump_trace() {
  for_each(g_debug_lst.begin(), g_debug_lst.end(), fn_print_string);
}
