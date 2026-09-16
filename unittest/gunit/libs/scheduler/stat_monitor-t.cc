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

#include <gtest/gtest.h>
#include <cstring>
#include <random>
#include <sstream>

#include "mysql/scheduler/delayed_schedule.h"
#include "mysql/scheduler/schedule_factory.h"
#include "mysql/scheduler/scheduler.h"
#include "mysql/scheduler/scheduler_clock.h"
#include "mysql/scheduler/statistics_map.h"
#include "mysql/scheduler/task_sequencer.h"

using namespace std;

namespace mysql::scheduler {
namespace {

void update1() {
  auto &stat_monitor = Statistics_monitor::get(0);
  stat_monitor.get_stat("a").add(-1, 0);
  stat_monitor.get_stat("b").add(6, 1);
}

void update2() {
  auto &stat_monitor = Statistics_monitor::get(0);
  stat_monitor.get_stat("a").add(2, 1);
  stat_monitor.get_stat("a").add(3, 4);
}

}  // namespace

TEST(Scheduler, StatisticsMonitor) {
  auto &stat_monitor = Statistics_monitor::get(0);
  stat_monitor.register_stat("a", 5);
  stat_monitor.register_stat("b", 4);
  update1();
  update2();
  ASSERT_EQ(stat_monitor.get_stat("a").get(), 4);
  ASSERT_EQ(stat_monitor.get_stat("a").get(1), 2);
  ASSERT_EQ(stat_monitor.get_stat("b").get(), 6);
}

}  // namespace mysql::scheduler
