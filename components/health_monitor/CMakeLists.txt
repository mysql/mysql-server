# Copyright (c) 2020, 2026, Oracle and/or its affiliates.
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

IF(NOT (LINUX OR APPLE))
  MESSAGE(STATUS
    "Health Monitor component is disabled because this platform is unsupported")
  RETURN()
ENDIF()

DISABLE_MISSING_PROFILE_WARNING()

SET(HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER "" CACHE STRING
  "Additional account protected from Health Monitor forced termination")
IF(HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER AND
    NOT "${HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER}" MATCHES
      "^[A-Za-z0-9_.-]+$")
  MESSAGE(FATAL_ERROR
    "HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER contains invalid characters")
ENDIF()

MYSQL_ADD_COMPONENT(health_monitor
  fx_holt_winters_forecaster.cc
  fx_stats_util.cc
  health_monitor.cc
  health_monitor_collector.cc
  health_monitor_cpu_mapping.cc
  health_monitor_filesystem_sample.cc
  health_monitor_memory_mapping.cc
  health_monitor_process_memory_mapping.cc
  health_monitor_process_stats_mapping.cc
  health_monitor_storage_mapping.cc
  health_monitor_disk.cc
  health_monitor_disk_forecast_reset.cc
  health_monitor_disk_forecaster.cc
  health_monitor_disk_forecaster_core.cc
  health_monitor_disk_forecaster_mode.cc
  health_monitor_disk_forecaster_report.cc
  health_monitor_disk_forecaster_test.cc
  health_monitor_disk_state.cc
  health_monitor_lock.cc
  health_monitor_log.cc
  health_monitor_memory.cc
  health_monitor_metrics.cc
  health_monitor_memory_state.cc
  health_monitor_option_usage.cc
  health_monitor_periodic_reporter.cc
  health_monitor_query.cc
  health_monitor_report_manager.cc
  health_monitor_server.cc
  health_monitor_status_vars.cc
  health_monitor_supervisor.cc
  health_monitor_system.cc
  health_monitor_system_state.cc
  health_monitor_system_vars.cc
  health_monitor_thread.cc
  health_monitor_timer.cc
  health_monitor_tables.cc
  health_monitor_triggered_reporter.cc
  health_monitor_util.cc
  table_base.cc
  table_disk_forecast.cc
  table_disk_forecast_summary.cc
  table_disk_metric.cc
  table_disk_metric_aggregate.cc
  table_health_block_device.cc
  table_health_process_memory.cc
  table_health_system_memory.cc
  table_system_cpu_stats.cc
  table_system_cpu_stats_base.cc
  table_system_cpu_stats_history.cc
  table_system_disk_stats.cc
  table_system_disk_stats_base.cc
  table_system_disk_stats_history.cc
  table_system_memory_stats.cc
  table_system_memory_stats_base.cc
  table_system_memory_stats_history.cc
  table_system_process_stats.cc
  table_system_process_stats_base.cc
  table_system_process_stats_history.cc

  MODULE_ONLY
  LINK_LIBRARIES library_mysys extra::rapidjson
  )

IF(HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER)
  TARGET_COMPILE_DEFINITIONS(component_health_monitor PRIVATE
    HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER=\"${HEALTH_MONITOR_ADDITIONAL_PROTECTED_USER}\")
ENDIF()

IF(WITH_UNIT_TESTS)
  MYSQL_ADD_EXECUTABLE(health_monitor_util-t
    gunit/health_monitor_util-t.cc
    ADD_TEST health_monitor_util
    LINK_LIBRARIES
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_configuration-t
    gunit/health_monitor_configuration-t.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_cpu_adapter-t
    gunit/health_monitor_cpu_adapter-t.cc
    health_monitor_cpu_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_memory_adapter-t
    gunit/health_monitor_memory_adapter-t.cc
    health_monitor_memory_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_host_memory_adapter-t
    gunit/health_monitor_host_memory_adapter-t.cc
    health_monitor_memory_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_filesystem_adapter-t
    gunit/health_monitor_filesystem_adapter-t.cc
    health_monitor_filesystem_sample.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_disk_forecast_reset-t
    gunit/health_monitor_disk_forecast_reset-t.cc
    fx_holt_winters_forecaster.cc
    fx_stats_util.cc
    health_monitor_disk_forecast_reset.cc
    health_monitor_lock.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_process_memory_adapter-t
    gunit/health_monitor_process_memory_adapter-t.cc
    health_monitor_process_memory_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_process_threads_adapter-t
    gunit/health_monitor_process_threads_adapter-t.cc
    health_monitor_process_stats_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )

  MYSQL_ADD_EXECUTABLE(health_monitor_storage_adapter-t
    gunit/health_monitor_storage_adapter-t.cc
    health_monitor_storage_mapping.cc
    ADD_TEST health_monitor
    LINK_LIBRARIES
    library_mysys
    gtest
    gtest_main
    server_unittest_library
    )
ENDIF()

ADD_DEFINITIONS(-DLOG_SUBSYSTEM_TAG="Health")
ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="Health")
