/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

// Make sure the C++11 library features are available
// Note that this file is meant for testing purposes only.

#include <typeindex>
#include <type_traits>
#include <chrono>
#include <initializer_list>
#include <tuple>
#include <cstdint>
#include <cinttypes>
#include <system_error>
//#include <cuchar>
#include <array>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <ratio>
#include <cfenv>
//#include <codecvt>
#include <regex>
#include <atomic>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>


long long cpp11_re_match(std::string pat, std::string arg)
{
  std::regex grepper(pat, std::regex_constants::egrep);

  return std::regex_match(arg, grepper);
}
