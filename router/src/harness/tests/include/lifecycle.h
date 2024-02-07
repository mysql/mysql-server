/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef LIFECYCLE_INCLUDED
#define LIFECYCLE_INCLUDED

#include <condition_variable>
#include <mutex>
#include <string>

namespace mysql_harness {
namespace test {

namespace PluginDescriptorFlags {
constexpr int NoInit = 1 << 0;
constexpr int NoDeinit = 1 << 1;
constexpr int NoStart = 1 << 2;
constexpr int NoStop = 1 << 3;
}  // namespace PluginDescriptorFlags

struct LifecyclePluginSyncBus {
  std::condition_variable cv;
  std::mutex mtx;
  std::string msg;
};

}  // namespace test
}  // namespace mysql_harness

#endif /* LIFECYCLE_INCLUDED */
