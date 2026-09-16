# Copyright (c) 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SET(TARGET_HEADERS
  base_dependency_tracker.h
  delayed_schedule.h
  dependency_tracker.h
  dependency_tracker_stub.h
  logger_stream.h
  schedule_factory.h
  scheduled_task.h
  scheduler.h
  scheduler_clock.h
  scheduler_impl.hpp
  sharded_counter.h
  task_id.h
  task_registry.h
  task_result.h
  task_schedule.h
  task_sequencer.h
  thread_pool.h
  thread_pool_impl.hpp
  time.h
  transaction_order_schedule.h
)

SET(TARGET_SRCS
  clock_lwm_registry.cpp
  commit_order_clock.cpp
  delayed_schedule.cpp
  schedule_factory.cpp
  scheduled_task.cpp
  scheduler.cpp
  sharded_counter.cpp
  statistics_instance_monitor.cpp
  statistics_map.cpp
  statistics_monitor.cpp
  task_id.cpp
  task_schedule.cpp
  transaction_order_schedule.cpp
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_scheduler
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
  COMPILE_DEFINITIONS STANDALONE_LIBS_MYSQL
  LINK_LIBRARIES ${LINK_LIBRARIES} mysql_concurrency extra::unordered_dense
)

set_target_properties(mysql_scheduler PROPERTIES LINKER_LANGUAGE CXX)

LIBS_MYSQL_CREATE_LIBRARY(mysql_scheduler_server
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
  LINK_LIBRARIES ${LINK_LIBRARIES} mysql_concurrency_server
    extra::unordered_dense
  DEPENDENCIES GenError
)
