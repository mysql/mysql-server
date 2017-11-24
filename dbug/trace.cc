/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <list>
#include <mutex>
#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "my_dbug.h"

using std::chrono::high_resolution_clock;
using std::chrono::time_point;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

void log_prefix(std::ostream& out)
{
  time_point<high_resolution_clock> tp1;
  tp1 = high_resolution_clock::now();
  std::chrono::nanoseconds ns = duration_cast<nanoseconds>(tp1.time_since_epoch());
  std::thread::id thread_id = std::this_thread::get_id();

  out << "ts=" << ns.count() << ", thread=" << thread_id << ": ";
}

static std::mutex g_mutex;
static std::list<std::string> g_debug_lst;
static const unsigned int MAX_NOTES = 20000;

void trace(const std::string& note)
{
  std::lock_guard<std::mutex> guard(g_mutex);

  g_debug_lst.push_back(note);

  if (g_debug_lst.size() > MAX_NOTES) {
    g_debug_lst.pop_front();
  }
}

void fn_print_string(const std::string& m) {
  std::cout << m << std::endl;
}

void dump_trace()
{
  for_each(g_debug_lst.begin(), g_debug_lst.end(), fn_print_string);
}

